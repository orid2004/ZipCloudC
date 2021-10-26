# Zip Cloud Service
A secure socket program, allowing file transferring and compressing. 
The program comes in handy when uploading large directories to a cloud,
without the need to modify them online. This feature allows clients 
to back up their entire `Desktop` and secure their data, 
while using limited cloud storage.

The project was written while reading the 
[Head first C book](https://www.oreilly.com/library/view/head-first-c/9781449335649/), as a final project of chapter 11- 
`"sockets and networking"`

## Languages
C  
Python

## Date
Feb 2021


## Description

The server allows clients to connect on a known port, 
and process a request. It can handle multiple clients simultaneously, and process requests.
The program prints output accordingly. 

Communication is composed of 3 steps:
* A request
* Data transfer (optional)
* A response

Requests are composed of 3 fields:
* Type
* ID
* Directory name (optional) 

New users can register an account by processing their first `upload` request
with a unique ID. A new ID, i.e. login information, can be created by the
`auth` command. The login credentials are saved locally and serve clients
within each request. The only way to access a registered account is by using 
the password that was used to register it. Passwords cannot be changed or
recovered.

Registered users can `tree` their account, `download`, `upload`, or `rm` 
directories. Each request must have a request type, followed by an ID 
and a directory name if necessary. 

The cloud compress new data, therefore directories can be only modified or 
replaced  locally using the `download` and the `rm` requests.

## Project Content
### Server
```
.
├── build
│   └── CMakeLists.txt
├── certificates
│   ├── servercert.pem
│   ├── serverkey.pem
│   └── serverreq.pem
├── connection.c
├── exit.c
├── include
│   ├── connection.h
│   ├── exit.h
│   ├── path.h
│   └── settings.h
├── main.c
├── path.c
├── sc.conf
├── source
│   └── walk.py
└── walk
```

### Client
```
.
├── build
│   └── CMakeLists.txt
├── connection.c
├── exit.c
├── include
│   ├── connection.h
│   ├── exit.h
│   ├── path.h
│   └── settings.h
├── main.c
├── path.c
├── source
│   └── walk.py
└── walk
```


## Usage
### Building
Link the source code and the `OpenSSL` library.
```
apt-get install libssl-dev
cmake build
make
```

### Server
The server requires a configuration file, 
which should contain a `server-key` and a `server-certificate`. 
I've generated the keys under the `server/certificates`
folder. 

Here is an example of a configuration file.
```
cert=certificates/servercert.pem
key=certificates/serverkey.pem
```

`./server sc.conf`

[How to generate new keys?](https://stackoverflow.com/questions/13295585/openssl-certificate-verification-on-linux)


### Auth
The `auth` command is used to create a new `ID`.
After running the `auth` command, the login credentials are saved locally and will
be used to authenticate with the server.

```
./client auth
Username: example_username
Password: pass123
```
New users must create a new `ID` file to be registered. Registered 
users must keep their `ID` file to access their account. The `ID` file 
is saved under the `App Data` directory, and shouldn't be changed manully by the user.

### Deauth
The `deauth` command is used to delete the current `ID` file.   
Note: A user cannot use the program without an `ID`.

```./client deauth```


### Upload
The `upload` command is used to upload directories to the cloud. New users will be registered by the server after sending their first `upload` request.

```
./client upload relative/Tetris
./client upload /Absolute/Tetris
```

### Download
The `download` command is used to download directories from the cloud.
The command accepts an output directory as an optional argument.
```
   ./client download Tetris
   ./client download Tetris to/relative
   ./client download Tetris /tmp/to/Absolute
```

### Tree
The `tree` command is used to list an account.  

`./client tree`

```
orid2004
|___ PycharmProjects
|___ Tetris
2 Folders
```

### Remove
The `rm` command is used to remove a directory. A user
confirmation is required.

`./client rm Tetris`

Passing `_` as a target directory is used to delete an
entire account. A user confirmation is required.

`./client rm _`


The `rm` command has no output. The `tree` command can be used to 
determinate if a directory, or an account, were indeed deleted.

## More Usage
### Rename
Renaming a cloud directory.
```
./client download Tetris
cp -R Tetris Tetris2
./client rm Tetris
./client upload Tetris2
```

### Replace
Replacing a cloud directory.
```
./client download Tetris
./client rm Tetris
./client upload Other
```

### Add File
Adding a new file to a cloud directory.
```
./client download Tetris
cp ~/file Tetris/path/to
./cient rm Tetris
./client upload Tetris
```

### Remove File
Removing a file from a cloud directory.
```
./client download Tetris
rm -f Tetris/path/to/file
./client rm Tetris
./client upload Tetris
```


## Performance
`Upload`

Size | 25 Mb | 256 Mb | 1024 Mb
--- | --- | --- | ---
Files | 843 | 6013 | 17137
Time | 3.26s | 34.16s | 224.34s
Zip Size | 27% | 64% | 70%


## Credits
Ori David  
 
## Contact
orid2004@gmail.com  
