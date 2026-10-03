function path_dirname(path, dir) {
    dir = path
    if (sub(/\/[^/]*$/, "", dir)) {
        if (dir == "") {
            return "/"
        }
        return dir
    }
    return "."
}

function file_exists(path, status, line) {
    if (path in exists_cache) {
        return exists_cache[path]
    }

    status = getline line < path
    close(path)
    exists_cache[path] = status >= 0
    return exists_cache[path]
}

function enqueue(path) {
    if (path == "" || queued[path]) {
        return
    }
    queued[path] = 1
    queue[queue_len] = path
    queue_len += 1
}

function resolve_include(source_file, include_file, source_dir,
                         candidate) {
    source_dir = path_dirname(source_file)
    candidate = source_dir "/" include_file
    if (file_exists(candidate)) {
        return candidate
    }
    if (file_exists(include_file)) {
        return include_file
    }
    return ""
}

function scan_line(source_file, line, include_file, resolved_file) {
    if (line !~ /^[[:space:]]*#[[:space:]]*include[[:space:]]*"/) {
        return
    }

    include_file = line
    sub(/^[^"]*"/, "", include_file)
    sub(/".*$/, "", include_file)
    if (include_file !~ /\.[ch]$/) {
        return
    }

    resolved_file = resolve_include(source_file, include_file)
    if (resolved_file != "") {
        enqueue(resolved_file)
    }
}

function scan_file(source_file, status, line) {
    if (scanned[source_file]) {
        return
    }

    status = getline line < source_file
    if (status < 0) {
        close(source_file)
        exists_cache[source_file] = 0
        return
    }

    scanned[source_file] = 1
    exists_cache[source_file] = 1
    print source_file

    if (status > 0) {
        scan_line(source_file, line)
    }
    while ((status = getline line < source_file) > 0) {
        scan_line(source_file, line)
    }
    close(source_file)
}

BEGIN {
    queue_len = 0

    for (arg_idx = 1; arg_idx < ARGC; arg_idx += 1) {
        enqueue(ARGV[arg_idx])
        ARGV[arg_idx] = ""
    }

    for (queue_idx = 0; queue_idx < queue_len; queue_idx += 1) {
        scan_file(queue[queue_idx])
    }
}
