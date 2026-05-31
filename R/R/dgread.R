#' dgread: Read Dynamic Group (dg/dgz) Data Files
#'
#' Fast reader for dg (dynamic group) and dgz (gzip-compressed) data files.
#' These files store collections of named arrays with support for nested and
#' ragged (variable-length) data, decompressed in memory (no temp file).
#'
#' @useDynLib dgread, .registration = TRUE
#' @importFrom tcltk .Tcl tclvalue
#' @keywords internal
"_PACKAGE"

#' Read a dg/dgz data file
#'
#' Reads a dynamic-group data file -- plain \code{.dg}, gzip-compressed
#' \code{.dgz}, or LZ4 \code{.lz4} -- into a named list. Each element is a
#' column; nested / ragged columns are returned as lists.
#'
#' @param file Path to the \code{.dg}/\code{.dgz}/\code{.lz4} file.
#' @param convert.underscore If \code{TRUE}, replace \code{_} with \code{.}
#'   in element names.
#' @return A named list of the group's columns.
#' @examples
#' d <- read.dgz(system.file("extdata", "sample.dgz", package = "dgread"))
#' names(d)
#' @export
"read.dgz" <-
  function (file, convert.underscore = FALSE) {
    rval <- .External("dgRead", file, PACKAGE = "dgread")

    if (convert.underscore)
      names(rval) <- gsub("_", ".", names(rval))
    return(rval)
  }

#' @rdname read.dgz
#' @export
"read.dg" <-
  function (file, convert.underscore = FALSE) {
    read.dgz(file, convert.underscore)
  }

"dg.read" <-
  function (file, convert.underscore = FALSE) {
    read.dgz(file, convert.underscore)
  }

"dg.fromString" <-
  function (s, convert.underscore = FALSE) {
    rval <- .External("dgFromString64", s, PACKAGE = "dgread")

    if (convert.underscore)
      names(rval) <- gsub("_", ".", names(rval))
    return(rval)
  }

"dg.exists" <-
  function(groupname) {
    .Tcl("package require dlsh")
    cmd <- paste("dg_exists", groupname)
    dgexists <- as.logical(.Tcl(cmd))
    return(dgexists)
  }

#' Fetch a dynamic group from a running dlsh/Tcl interpreter
#'
#' Retrieves a dynamic group by name from an embedded dlsh Tcl interpreter
#' (via the \pkg{tcltk} bridge) and returns it as a named list.
#'
#' @param groupname Name of the dynamic group in the Tcl interpreter.
#' @param convert.underscore If \code{TRUE}, replace \code{_} with \code{.}
#'   in element names.
#' @return A named list of the group's columns, or \code{NULL} if the group
#'   does not exist.
#' @export
"dg.get" <-
  function(groupname, convert.underscore = FALSE) {
    if (dg.exists(groupname)) {
      tmpnam <- "__dg_toString__"
      .Tcl(paste("dg_toString64", groupname, tmpnam))
      dgstring <- tclvalue(tmpnam)
      .Tcl(paste("unset", tmpnam))
      ans <- dg.fromString(dgstring)

      if (convert.underscore)
        names(ans) <- gsub("_", ".", names(ans))
      return(ans)
    }
  }

"dg.write" <-
  function(sexp, name) {
    rval <- .External("dgWrite", sexp, name, PACKAGE = "dgread")
    return (rval)
  }
