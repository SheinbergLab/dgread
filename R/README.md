# dgread - R

R package for reading dg/dgz dynamic group data files.

## Installation

### From Source

```r
# Install from local clone
install.packages("path/to/dgread/R", repos = NULL, type = "source")

# Or using devtools
devtools::install_local("path/to/dgread/R")
```

### Requirements

- R >= 3.5
- C compiler (Rtools on Windows, Xcode on macOS, gcc on Linux)
- zlib development library

On Linux:
```bash
sudo apt install zlib1g-dev  # Debian/Ubuntu
sudo yum install zlib-devel  # RHEL/CentOS
```

## Usage

```r
library(dgread)

# Read a dgz file
data <- read.dgz("session.dgz")

# Access as list elements
mean(data$rt)
sd(data$response)

# Ragged data
for (i in 1:3) {
  cat(sprintf("Trial %d: %d samples\n", i, length(data$em[[i]])))
}
```

## Functions

### read.dgz / read.dg

```r
data <- read.dgz(filename)
data <- read.dg(filename)
```

Read a dg or dgz file and return a named list.

### dg.get

```r
values <- dg.get(data, "fieldname")
```

Extract a specific field from loaded dg data.

## Data Types

| dg Type | R Type |
|---------|--------|
| Integer | integer |
| Float | numeric |
| String | character |
| List (nested) | list |

## Building

The R package uses shared C sources from `../src/`. The `Makevars` file handles the include paths.

To build manually:

```bash
cd R
R CMD build .
R CMD INSTALL dgread_1.1.0.tar.gz
```

To check:

```bash
R CMD check dgread_1.1.0.tar.gz
```
