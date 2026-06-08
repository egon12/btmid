use std::convert::TryInto;
use std::env;
use std::f32::consts::PI;
use std::fs::File;
use std::io::{BufReader, Read};

fn fft(input: Vec<f32>) -> Vec<f32> {
    let freq_from = 40;
    let freq_to = 24_000;

    let mut output = vec![0.0; freq_to - freq_from];

    for freq in freq_from..freq_to {
        let frame_rate = 48_000;
        let total_samples = input.len();

        let mut sum_real = 0.0;
        let mut sum_imag = 0.0;
        let freq_f = freq as f32;

        for i in 0..total_samples {
            let time: f32 = i as f32 / frame_rate as f32;
            let phase: f32 = 2.0 * PI * freq_f * time;
            sum_real += phase.cos() * input[i];
            sum_imag += phase.sin() * input[i];
        }

        let magnitude = (sum_real * sum_real + sum_imag * sum_imag).sqrt();

        output[freq - freq_from] = magnitude;
    }

    return output;
}

fn main() {
    let filename = match env::args().nth(1) {
        Some(filename) => filename,
        None => {
            println!("No filename provided.");
            return;
        }
    };

    let file = File::open(filename).expect("failed to open file");
    let mut file = BufReader::new(file);

    let mut header_buf = [0u8; 44];
    file.read_exact(&mut header_buf).unwrap();

    let mut data = vec![0; 48_000 * 4 as usize];
    file.read_exact(&mut data).unwrap();
    let d: Vec<f32> = data
        .chunks_exact(2)
        .map(|x| i16::from_le_bytes(x.try_into().unwrap()))
        .map(|x| x as f32 / 32768.0)
        .collect();

    println!("Start FFT");
    let output = fft(d);

    println!("Frequency\tMagnitude");
    let mut index = 1;
    let mut sum = 0.0;
    let mut count = 0;
    let mut start_freq = 0.0;
    for i in 40..24_000 {
        let idx = i as f32;
        let fl = (idx.log2() * 100.0).floor() as usize;
        if fl == index {
            sum += output[i-40];
            count += 1;
        } else {
            println!("{}: {}", start_freq, sum / count as f32);
            index = fl;
            sum = 0.0;
            count = 0;
            start_freq = idx;
        }
    }

    //output.iter().for_each(|x| println!("{}", x));
}
