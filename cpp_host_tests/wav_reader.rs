use std::convert::TryInto;
use std::env;
use std::fs::File;
use std::io::{BufReader, Read};
use std::path::Path;
use std::thread;
use std::time::Duration;

pub struct WavReader {
    file: BufReader<File>,
    header: WavHeader,
    data: Vec<f32>,
}

#[derive(Debug)]
struct WavHeader {
    riff: [u8; 4],
    size: u32,
    wave: [u8; 4],
    fmt: [u8; 4],
    fmt_size: u32,
    audio_format: u16,
    num_channels: u16,
    sample_rate: u32,
    byte_rate: u32,
    block_align: u16,
    bits_per_sample: u16,
    data: [u8; 4],
    data_size: u32,
}

impl WavReader {
    pub fn new<P: AsRef<Path>>(path: P) -> Self {
        let mut file = File::open(path).expect("Failed to open file");
        let mut file = BufReader::new(file);

        let mut header_buf = [0u8; 44];
        file.read_exact(&mut header_buf).unwrap();

        let header = WavHeader {
            riff: header_buf[0..4].try_into().unwrap(),
            size: u32::from_le_bytes(header_buf[4..8].try_into().unwrap()),
            wave: header_buf[8..12].try_into().unwrap(),
            fmt: header_buf[12..16].try_into().unwrap(),
            fmt_size: u32::from_le_bytes(header_buf[16..20].try_into().unwrap()),
            audio_format: u16::from_le_bytes(header_buf[20..22].try_into().unwrap()),
            num_channels: u16::from_le_bytes(header_buf[22..24].try_into().unwrap()),
            sample_rate: u32::from_le_bytes(header_buf[24..28].try_into().unwrap()),
            byte_rate: u32::from_le_bytes(header_buf[28..32].try_into().unwrap()),
            block_align: u16::from_le_bytes(header_buf[32..34].try_into().unwrap()),
            bits_per_sample: u16::from_le_bytes(header_buf[34..36].try_into().unwrap()),
            data: header_buf[36..40].try_into().unwrap(),
            data_size: u32::from_le_bytes(header_buf[40..44].try_into().unwrap()),
        };

        let data = WavReader::read_data(&mut file, &header);
        Self { file, header, data }
    }

    fn read_data<R: Read>(reader: &mut R, header: &WavHeader) -> Vec<f32> {
        let mut data = vec![0; header.data_size as usize];
        reader.read_exact(&mut data).unwrap();
        data.chunks_exact(2)
            .map(|x| i16::from_le_bytes(x.try_into().unwrap()))
            .map(|x| x as f32 / 32768.0)
            .collect()
    }
}

fn main() {
    let filename = match env::args().nth(1) {
        Some(filename) => filename,
        None => {
            println!("No filename provided.");
            return;
        }
    };

    let wav = WavReader::new(filename);
    //println!("{:?}", wav.header);
    wav.data.iter().for_each(|x| {
        println!("{}", x);
        thread::sleep(Duration::from_millis(5));
    });
}
