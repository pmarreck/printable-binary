// Minimal CLI for cross-impl verification + the transport smoke path.
// (Provisional surface — the full CLI is a "specifics" decision still pending.)
// Default: encode stdin->stdout. `-d`/`--decode`: decode stdin->stdout.
use printable_binary::{decode, encode};
use std::io::{Read, Write};

fn main() {
    let decode_mode = std::env::args().skip(1).any(|a| a == "-d" || a == "--decode");
    let mut input = Vec::new();
    std::io::stdin().read_to_end(&mut input).expect("read stdin");
    let out = if decode_mode { decode(&input) } else { encode(&input) };
    std::io::stdout().write_all(&out).expect("write stdout");
}
