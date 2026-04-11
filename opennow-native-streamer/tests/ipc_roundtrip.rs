use opennow_native_streamer::ipc::{encode_frame, ControlMessage, FrameDecoder};

#[test]
fn decodes_multiple_framed_messages() {
    let hello = encode_frame(&ControlMessage::Ping).unwrap();
    let stop = encode_frame(&ControlMessage::StopSession { reason: Some("done".into()) }).unwrap();
    let mut decoder = FrameDecoder::default();
    let combined = [hello, stop].concat();
    decoder.push(&combined);
    assert_eq!(decoder.try_next::<ControlMessage>().unwrap(), Some(ControlMessage::Ping));
    assert_eq!(decoder.try_next::<ControlMessage>().unwrap(), Some(ControlMessage::StopSession { reason: Some("done".into()) }));
}
