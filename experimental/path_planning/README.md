## General Project Structure
```
./
├── src/                Contains all the source code
│ ├── path_planning/    Contains all the path-planning algorithms
├── tests/              Contains all the tests for source code
│ ├── mazes/            Temporary storage location for generated mazes
├── build/              Contains all the object files before linking them
├── Makefile            Compiles the project for you
├── main.exe            The final executable generated from the Makefile
├── .gitignore          Contains directories to ignore
└── README.md
```

## How to use Makefile
There are a few primary commands to be aware of:
* make setup: Sets up the necessary directories described in the project structure
* "make"/"make all": Builds the entire project and generates an executable called "main.exe" in "./"
* make clean: Removes all the build files and the generated mazes.
