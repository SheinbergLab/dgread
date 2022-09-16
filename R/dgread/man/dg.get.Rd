\name{dg.get}
\alias{dg.get}
\title{Bring dg from tcl to R}
\description{
  Dynamic groups are found and imported from the tcl to R
}
\usage{
dg.get(groupname, convert.underscore=FALSE)
}
\arguments{
  \item{groupname}{a dg in the tcltk interpreter}
  \item{convert.underscore}{Convert \code{"_"} in dgz dynlist names
    to \code{"."} in R names?}
}
\details{
  dg.get looks for an existing group in the tcl environment and if it
  exists, it writes it to a temp file (using dg_write) and reads it back
  (using dg.read).  The result is an R list.
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
  dg.get("group1")
}
\keyword{file}
