# Korg M3 Helper
This is a small console program I wrote to help with some annoying repetitive tasks on the [Korg M3][m3] keyboard. When preparing for a show, I copy the combis I need, in order, to a particular bank. Doing this for up to 60 combies, trying to keep track of where I'm up to, is incredibly tedious, time-consuming, and error-prone. This little utility allows me to copy all the combis in sequencce by just entering the source combi numbers in order.

## Building
* To cross-build for Windows using Linux:
  * Ensure you have the MinGW-W64 g++ compiler installed:
    ```shell
    sudo apt-get install g++-mingw-w64 # debian
    ```
  * `make PLATFORM=windows`
* To build for Linux:
  * ```shell
    make PLATFORM=unix
    ```
    The platform should default to unix if you are on a unix-based system anyway.
* To build on Windows:
  * Load the Visual Studio Solution
  * Build Solution


## Using
* Run the program from the build directory, e.g., `./bin/Windows/Debug/M3.exe`
* The program should automatically detect the M3 if it is connected via USB. Otherwise, it will list all the available MIDI inputs and outputs for you to choose.
* Type 'help' for instructions in the program.
* Type 'exit' or 'quit' to close the program.

### Example
To copy a bunch of combis from bank U-F to U-G, starting at U-G030
```
> mode combi
> copysrc U-F
> copydest U-G
> copyseq 30
< 24
< 38
< 21
<
<
< 8
< 0
< exit
> exit
```
This will copy combis U-F024, U-F038, U-F021, U-F022, U-F023, U-F008, U-F000 into U-G030..U-G036




[m3]: https://www.korg.com/us/support/download/product/1/140/
