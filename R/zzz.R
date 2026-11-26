.onLoad = function(libname, pkgname) {
  dll = getLoadedDLLs()[[pkgname]]
  reg.finalizer(
    environment(.onLoad),
    function(ns) {
      if (identical(dll, getLoadedDLLs()[[pkgname]])) {
        .Call(ns$C_vdb_join_workers)
      }
    },
    onexit = TRUE
  )
}

.onUnload = function(libpath) {
  .Call(C_vdb_join_workers)
}
