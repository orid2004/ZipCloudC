from pathlib import Path
import sys


def main():
    path = Path(sys.argv[1])

    with open("tmp", "w") as f:
        f.write(str(path.resolve()))


if __name__ == '__main__':
    main()
