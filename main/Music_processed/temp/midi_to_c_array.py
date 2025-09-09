import mido
import os

def midi_to_c_array(filename):
    try:
        mid = mido.MidiFile(filename)
    except Exception as e:
        print(f"Error opening MIDI file {filename}: {e}")
        return

    melody = []      # 频率数组
    durations = []   # 持续时间（毫秒）
    
    ticks_per_beat = mid.ticks_per_beat if mid.ticks_per_beat else 480
    tempo = 500000  # 默认 120 BPM

    # 尝试获取第一个 set_tempo 事件
    for track in mid.tracks:
        for msg in track:
            if msg.type == 'set_tempo' and msg.is_meta:
                tempo = msg.tempo
                break
        if tempo != 500000:
            break

    # 用于跟踪每个音符的开始时间（支持同音符重叠）
    active_notes = {}  # key: (note, channel), value: start_tick

    for track in mid.tracks:
        current_tick = 0
        for msg in track:
            current_tick += msg.time  # 累计时间

            if msg.type == 'note_on' and msg.velocity > 0:
                key = (msg.note, msg.channel)
                # 记录这个音符的开始时间（tick）
                active_notes[key] = current_tick

            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                key = (msg.note, msg.channel)
                if key in active_notes:
                    start_tick = active_notes.pop(key)
                    duration_ticks = current_tick - start_tick
                    duration_ms = int(mido.tick2second(duration_ticks, ticks_per_beat, tempo) * 1000)
                    
                    frequency = int(440 * (2 ** ((msg.note - 69) / 12)))
                    melody.append(frequency)
                    durations.append(duration_ms)

    if not melody:
        print(f"Warning: No notes found in {filename}")
        return

    # 生成头文件
    base_name = os.path.splitext(os.path.basename(filename))[0]
    output_filename = f"{base_name}.h"
    try:
        with open(output_filename, 'w') as f:
            f.write("const int melody[] PROGMEM= {\n")
            for i, note in enumerate(melody):
                f.write(f"  {note},")
                if (i + 1) % 10 == 0:
                    f.write("\n")
                else:
                    f.write(" ")
            f.write("\n};\n\n")

            f.write("const int durations[] PROGMEM= {\n")
            for i, duration in enumerate(durations):
                f.write(f"  {duration},")
                if (i + 1) % 10 == 0:
                    f.write("\n")
                else:
                    f.write(" ")
            f.write("\n};\n")

            f.write(f"\n// Song length: {len(melody)} notes\n")
            f.write(f"// Generated from {filename}\n")

        print(f"✅ Successfully wrote: {output_filename} ({len(melody)} notes)")
    except Exception as e:
        print(f"❌ Error writing {output_filename}: {e}")

if __name__ == "__main__":
    current_dir = "."
    midi_files = [f for f in os.listdir(current_dir) if f.lower().endswith(('.mid', '.midi'))]

    if not midi_files:
        print("No MIDI files found in the current directory.")
    else:
        print(f"Found {len(midi_files)} MIDI file(s):")
        for f in midi_files:
            print(f"  → {f}")
        print("-" * 40)

        for midi_file in midi_files:
            midi_to_c_array(midi_file)