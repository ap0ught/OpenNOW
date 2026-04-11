use std::{env, process};

fn main() {
    opennow_native_streamer::logging::init();
    let args: Vec<String> = env::args().collect();
    if args.iter().any(|arg| arg == "--help") {
        println!("opennow-native-streamer --ipc-endpoint <path>");
        return;
    }
    let endpoint = args.windows(2).find_map(|pair| if pair[0] == "--ipc-endpoint" { Some(pair[1].clone()) } else { None });
    if endpoint.is_none() {
        eprintln!("missing --ipc-endpoint");
        process::exit(2);
    }
    println!("OpenNOW Native Streamer bootstrap for {:?}", opennow_native_streamer::platform::TargetPlatform::current());
}
