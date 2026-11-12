# Security

The web dashboard (`web/server.py`) is meant for a local lab network. It has no passwords: anyone who can open the
page can watch the machine and press emergency stop, and only the PC running the server can start it or change the
speed.

## Please keep in mind

- Do not expose the server directly to the internet. If you use a tunnel to share it, remember that every viewer
  can stop the machine, and requests through a tunnel are treated as remote (view and stop only).
- `--allow-remote-control` gives every viewer full control. Use it only on a network you trust.
- The server accepts only a fixed list of commands (`g x z l s d + -` and numbers) and serves only its own files.

## Reporting a vulnerability

Please report security problems privately through
[GitHub security advisories](../../security/advisories/new) rather than in a public issue. Include what you did,
what happened, and how to reproduce it. You will get a reply within a week.
