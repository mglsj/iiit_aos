# a3: Peer-to-Peer File Sharing

Spec: [assignment_3.pdf](assignment_3.pdf)

Two synchronised trackers hold users, groups and file metadata. Clients download 512 KB pieces from several peers in parallel and verify each with SHA-1. Needs GCC 16 (reflection, contracts).

```sh
make build    # asan+ubsan in build/asan, tsan in build/tsan
build/asan/tracker tracker_info.txt <0|1>
build/asan/client <ip:port> tracker_info.txt
```

Client commands: `create_user`, `login`, `logout`, `create_group`, `join_group`, `leave_group`, `list_groups`, `list_requests`, `accept_request`, `upload_file`, `list_files`, `download_file`, `stop_share`, `show_downloads`.

Layout: `common/` protocol and shared code, `tracker/`, `client/`.
