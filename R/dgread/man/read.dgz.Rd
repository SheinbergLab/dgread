\name{read.dgz}
\alias{read.dg,dg.read}
\title{Read DLSH .dg/.dgz data files}
\description{
  Reads a file store from dlsh binary format into a S list. 
}
\usage{
read.dgz(file, convert.underscore=FALSE)
}
\arguments{
  \item{file}{a filename as a character string.}
  \item{convert.underscore}{Convert \code{"_"} in dgz dynlist names
    to \code{"."} in R names?}
}
\details{
  Each dynlist becomes a list in the output vector.  Lists of lists are
  converted as such.  Float lists become doubles, and short and chars
  become ints.  Strings and ints are converted as expected.
  
  Note that temporary files for decompressing gzip'd dgz file are
  necessary and will be created and deleted by this process.  The
  name of the temporary file is determined using the "tempnam" function.
}
\value{
  a list
}
\references{
  DLSH documentation
}

\author{David Sheinberg}
\seealso{
}
\examples{
  read.dgz("test.dgz")
}
\keyword{file}
