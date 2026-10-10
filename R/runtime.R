#' OpenVDB runtime version and native interface
#' @return `vdb_version()` returns the bundled OpenVDB version.
#'   `vdb_api()` returns an external pointer to the version-one C function table,
#'   retaining the package namespace for the lifetime of the handle.
#' @details Native consumers can use `LinkingTo: openvdbr` for the small C headers
#'   and `Imports: openvdbr` to ensure the provider is loaded. On R's main thread,
#'   call `openvdbr_api_get_v1()` from `openvdbr/openvdbr_r.h`. It retrieves
#'   `openvdbr_get_api_v1` using `R_GetCCallable()`. No OpenVDB linker flags or
#'   OpenVDB C++ headers are needed by the consumer.
#'
#'   The table uses opaque handles and caller-owned error buffers. Read-only
#'   sampling calls can run concurrently with separate output buffers. Mutating
#'   or closing a grid must not overlap any access to that grid. File handles
#'   require exclusive access. Runtime lookup and all R API calls must remain
#'   on R's main thread. Explicitly unloading the provider DLL while handles or
#'   consumers remain alive is unsupported.
#' @useDynLib openvdbr, .registration = TRUE
#' @importFrom Rcpp evalCpp
#' @export
vdb_version = function() .Call(C_vdb_version)

#' @rdname vdb_version
#' @export
vdb_api = function() .Call(C_vdb_api, asNamespace('openvdbr'))

#' Retrieve buffered library diagnostics
#'
#' Collect warnings and informational diagnostics from the bundled libraries,
#' including messages produced on worker threads. Operation failures still raise
#' ordinary R errors (or return a status to native consumers).
#' @return A character vector of pending messages, removed from the queue.
#' @details The queue is global to this provider. It retains up to 128 messages
#'   of 511 bytes each, then reports a count of dropped messages. This bounds
#'   memory on error paths and avoids console calls from worker threads.
#'   Native consumers can drain the same queue with `diagnostic_next()`.
#' @export
vdb_messages = function() .Call(C_vdb_messages)
