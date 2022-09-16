library(tcltk)

"read.dgz" <-
  function (file,convert.underscore=FALSE) {
    rval <- .External("dgRead", file, PACKAGE = "dgread")

    if(convert.underscore)
      names(rval)<-gsub("_",".",names(rval))
    return(rval)
  }

"read.dg" <-
  function (file,convert.underscore=FALSE) {
    read.dgz(file,convert.underscore)
  }

"dg.read" <-
  function (file,convert.underscore=FALSE) {
    read.dgz(file,convert.underscore)
  }

"dg.fromString" <-
  function (s, convert.underscore=FALSE) {
    rval <- .External("dgFromString64", s, PACKAGE = "dgread")

    if(convert.underscore)
      names(rval)<-gsub("_",".",names(rval))
    return(rval)
  }

"dg.exists" <-
  function(groupname) {
    .Tcl("package require dlsh")
    cmd <- paste("dg_exists",groupname)
    dgexists <- as.logical(.Tcl(cmd))
    return(dgexists)
  }

"dg.get" <-
  function(groupname, convert.underscore=FALSE) {
    if (dg.exists(groupname)) {
      tmpnam <- "__dg_toString__"
      .Tcl(paste("dg_toString64",groupname,tmpnam))
      dgstring <- tclvalue(tmpnam)
      .Tcl(paste("unset", tmpnam))
      ans <- dg.fromString(dgstring)

      if(convert.underscore)
        names(ans)<-gsub("_",".",names(ans))
      return(ans)
    }
  }
 
"dg.write" <-
  function(sexp, name) {
    rval <- .External("dgWrite", sexp, name, PACKAGE = "dgread")
    return (rval)
  }
