# IotaOS
A simple OS made for fun 

# Set Up GCC Cross Compiler

> [!NOTE]
> I have tried this before and for me I have not needed `build-essential` so bewere you may need gcc and make installed

1. Install Build Dependencies
```bash
sudo apt update
sudo apt install bison flex libgmp3-dev libmpc-dev libmpfr-dev libisl-dev

# For Texinfo (optional)
sudo apt install libncurses5-dev libncursesw5-dev
```
2. Install Legacy Texinfo (Optional)

> [!CAUTION]
> I would **definitely not** recommend manually installing an *ancient* version of `texinfo` like I am. I’m only doing this for the sake of learning the deep internals and following the OSDev Wiki to the letter. If you just want to build the OS, don't install `texinfo`! However, if you plan on changing core components, I would recommend it; it is a fairly fast install and might save time if you need to look up documentation for a feature you aren't familiar with.

2a. Install Texinfo (4.13)

```bash
mkdir -p ~/src
cd ~/src

# Download and unpack
wget https://ftp.gnu.org/gnu/texinfo/texinfo-4.13a.tar.gz
tar -xf texinfo-4.13a.tar.gz
cd texinfo-4.13

# Fix modern compiler security errors
sed -i 's/_GL_WARN_ON_USE (gets, "gets is a security hole - use fgets instead");/\/\/_GL_WARN_ON_USE (gets, "gets is a security hole - use fgets instead");/g' gnulib/lib/stdio.in.h

# Build and link against ncurses
./configure --prefix=$HOME/opt/texinfo4 LDFLAGS="-lncurses"
make
make install

# Add to path
export PATH="$HOME/opt/texinfo4/bin:$PATH"
```

2b. Verify Installation
```bash
makeinfo --version       # Should report 4.13
```

3. Install Binutils (2.23.2)

3a. Setup enviroment
> [!IMPORTANT]
> These variables are session-based. If you close your terminal, you must run these again before proceeding.
```bash
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
```

3b. Install Binutils (2.23.2)
```bash
cd ~/src
wget https://ftp.gnu.org/gnu/binutils/binutils-2.23.2.tar.gz
tar -xf binutils-2.23.2.tar.gz

mkdir build-binutils
cd build-binutils

../binutils-2.23.2/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror MAKEINFO=missing
make MAKEINFO=missing
make install MAKEINFO=missing
```

4. Install GCC (4.8.0)

```bash
cd ~/src
wget https://ftp.gnu.org/gnu/gcc/gcc-4.8.0/gcc-4.8.0.tar.gz
tar -xf gcc-4.8.0.tar.gz

mkdir build-gcc
cd build-gcc

# Configure for C and C++
../gcc-4.8.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers MAKEINFO=missing

# GCC build has two main targets: the compiler and the internal library
make all-gcc CXXFLAGS="-O2 -std=gnu++03" MAKEINFO=missing
make all-target-libgcc MAKEINFO=missing
make install-gcc CXXFLAGS="-O2 -std=gnu++03" MAKEINFO=missing
make install-target-libgcc MAKEINFO=missing
```
5. Verification <br>
Go to the repo root and run these:
```bash
i686-elf-gcc --version   # Should report 4.8.0
i686-elf-ld --version    # Should report 2.23.2
i686-elf-as --version    # Should report 2.23.2
```