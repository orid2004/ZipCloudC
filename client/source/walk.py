import os
import sys
import fileinput
import scandir
import threading
import time
from pathlib import Path
import queue
import mmap


MB = 1024*1024

def get_mb(path):
   return Path(path).stat().st_size / MB


def abs(path):
    return os.path.abspath(path)

def pjoin(*args):
    return os.path.join(*args)

def build_msg(l):
    return '\r\n\r\n'.join(l);


files_st, dirs_st, len_st = [], [], []
content = b""
count_mb = 0
count_files = 0
total_files = 0
content_tmp = None


def count_total_files(baseDir, root):
    global total_files
    for entry in scandir.scandir(baseDir):
        if entry.is_file():
            total_files += 1
        else:
            count_total_files(entry.path, root)

def log_return(msg, end="\n"):
    print(f"\033[A{' '*100}\033[A")
    print(msg, end=end)

def scanRecursive(baseDir, root):
    global files_st, dirs_st, len_st, count_mb, content, count_files, total_files, content_tmp
    for entry in scandir.scandir(baseDir):
        if entry.is_file():
            count_files += 1
            count_mb += get_mb(entry.path)
            files_st.append(entry.path.replace(abs(root) + '/', ''))
            data = b""
            with open(entry.path, mode="rb") as file_obj:
                try:
                    with mmap.mmap(file_obj.fileno(), length=0, access=mmap.ACCESS_READ) as mmap_obj:
                        data = mmap_obj.read()
                except:
                    pass
            content_tmp.write(data)
            len_st.append(str(len(data)))  # This would give length of files..
            log_return(f"\r[{round(count_mb, 1)}MB] [{count_files}/{total_files}] reading {entry.name}")
            sys.stdout.flush()
        else:
            dirs_st.append(entry.path.replace(abs(root) + '/', ''))
            scanRecursive(entry.path, root)

def get_meta(path):
    global files_st, dirs_st
    if not path.endswith('/'): path += '/'
    count_total_files(path, path)
    print("reading")
    scanRecursive(path,path)


def save_meta(outfile, root):
    global files_st, dirs_st, len_st, content_tmp
    content_tmp.close()
    with open(outfile, "wb") as f:
        if root.endswith('/'):
            folder_name = root.split('/')[-2]
        else:
            folder_name = root.split('/')[-1]
        top = build_msg((folder_name, '\r\n'.join(files_st), "\r\n".join(dirs_st), "\r\n".join(len_st))) + '\r\n\r\n'
        with open("/tmp/content.meta", 'rb') as content:
            f.write(bytes(top, encoding='utf8') + content.read())


def extract_all(meta, out_path):
    data_holder = ["",]*3
    dir_name = ""
    short_delim = "\r\n"
    delim = "\r\n\r\n"
    with open(meta, "rb") as src:
        print()
        log_return(f"reading meta...%")
        while not dir_name.endswith(delim):
            dir_name += src.read(1).decode("utf8")
        dir_name = dir_name[0:-4]

        for i in range(len(data_holder)):
            log_return(f"reading meta...")
            while not data_holder[i].endswith(delim):
                data_holder[i] += src.read(1).decode("utf8")
            data_holder[i] = data_holder[i][0:-4].split(short_delim)

        file_list, dir_list = data_holder[:2]
        len_pointers = [int(x) for x in data_holder[2]]
        out_dir = os.path.join(out_path, dir_name)
        if os.path.isdir(out_dir):
            print("Target directory is already exists inside the given path.")
            return
        else:
            os.mkdir(out_dir)
            for dir in dir_list:
                os.mkdir(os.path.join(out_dir, dir))
        size = len(len_pointers)
        for file_i in range(size):
            #log_return(f"reading meta... {round((file_i+1)*100/size, 1)}%")
            with open(os.path.join(out_dir, file_list[file_i]), "wb") as dst:
                print(f"created file {os.path.join(out_dir, file_list[file_i])}")
                dst.write(src.read(len_pointers[file_i]))

def main():
    global content_tmp
    if sys.argv[1] in ("-w", "--walk"):
        content_tmp = open("/tmp/content.meta", "wb+")
        s_time = time.time()
        get_meta(sys.argv[2])
        save_meta("/tmp/out.meta", (sys.argv[2]))
        os.remove('/tmp/content.meta')
        print(f"\r[{round(time.time() -s_time, 2)}s]")
        sys.stdout.flush()
    elif sys.argv[1] == '-d':
        extract_all(*sys.argv[2:4])
    else:
        print(f"Usage: {sys.argv[0]} <flag> <path>")

if __name__ == '__main__':
    main()
