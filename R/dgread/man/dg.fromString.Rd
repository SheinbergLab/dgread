\name{dg.fromString}
\alias{dg.fromString}
\title{Convert memory buffer to dynamic group}
\description{
  Given a character string, this function converts that into a SEXP
  representation representing the corresponding dynamic group from dlsh
}
\usage{
dg.fromString(s,convert.underscore=FALSE)
}
\arguments{
  \item{s}{a character vector continaing the string representation of
    the dg}
  \item{convert.underscore}{Convert \code{"_"} in dgz dynlist names
    to \code{"."} in R names?}
}
\details{
  A dynamic group that is converted to a string from dlsh using the
  dg_toString function can be loaded into R (using file or socket
  connections) and the buffer can be converted to an equivalent list in R.
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

}
\keyword{dg}
