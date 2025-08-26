import mido
import os

def analyze_track(msgs):
    """分析一个音轨的特征"""
    note_count = 0
    unique_notes = set()
    on_events = 0
    off_events = 0
    channels = set()
    has_drums = False

    for msg in msgs:
        if msg.type in ['note_on', 'note_off']:
            note_count += 1
            if hasattr(msg, 'channel'):
                channels.add(msg.channel)
            if msg.type == 'note_on':
                on_events += 1
                # MIDI 鼓通常在 channel 9（从0开始）
                if msg.channel == 9:
                    has_drums = True
            elif msg.type == 'note_off':
                off_events += 1
            if msg.type == 'note_on' and msg.velocity > 0:
                unique_notes.add(msg.note)

    # 简单判断是否像旋律：音符多、音高变化多、不是鼓
    is_melody_candidate = (
        note_count > 5 and
        len(unique_notes) > 3 and
        not has_drums and
        8 <= len(channels) <= 1  # 通常是1个channel，但有些文件可能多个
    )

    return {
        'note_count': note_count,
        'unique_notes': len(unique_notes),
        'channels': channels,
        'has_drums': has_drums,
        'is_melody_candidate': is_melody_candidate
    }

def analyze_midi_file(filename):
    try:
        mid = mido.MidiFile(filename)
    except Exception as e:
        print(f"❌ 无法读取 {filename}: {e}")
        return

    print(f"\n🎵 文件: {filename}")
    print(f"   Ticks per beat: {mid.ticks_per_beat}")
    print(f"   Tracks: {len(mid.tracks)}")
    print("-" * 80)

    candidates = []

    for i, track in enumerate(mid.tracks):
        track_name = getattr(track, 'name', f'Unnamed Track {i}').strip()
        if not track_name:
            track_name = f'Unnamed Track {i}'

        info = analyze_track(track)

        # 判断是否为鼓轨（名字或 channel 9）
        name_lower = track_name.lower()
        is_definitely_drums = 'drum' in name_lower or 'percussion' in name_lower or info['has_drums']

        # 标记建议
        if info['is_melody_candidate'] and not is_definitely_drums:
            suggestion = "✅ 候选主旋律"
            candidates.append((i, track_name))
        elif is_definitely_drums:
            suggestion = "🥁 鼓轨（跳过）"
        elif info['note_count'] == 0:
            suggestion = "🔇 无声轨（元数据）"
        else:
            suggestion = "🎼 伴奏/和弦（可选）"

        print(f"Track {i:2d}: '{track_name}'")
        print(f"         → 音符事件: {info['note_count']:3d} | 不同音高: {info['unique_notes']:2d} | "
              f"通道: {sorted(info['channels']) if info['channels'] else []} | {suggestion}")

    # 输出推荐
    print()
    if candidates:
        print(f"💡 推荐主旋律音轨: Track {candidates[0][0]} ('{candidates[0][1]}')")
        for i, (idx, name) in enumerate(candidates[1:], 1):
            print(f"             备选 {i}: Track {idx} ('{name}')")
    else:
        print("❌ 未找到合适的主旋律候选，请手动检查。")

def main():
    current_dir = "."
    midi_files = [f for f in os.listdir(current_dir)
                  if f.lower().endswith('.mid') or f.lower().endswith('.midi')]

    if not midi_files:
        print("🔍 当前目录没有找到 .mid 或 .midi 文件。")
        return

    print(f"🔍 发现 {len(midi_files)} 个 MIDI 文件：")
    for f in midi_files:
        print(f"  ➤ {f}")

    print("\n" + "="*80)
    print("📊 开始分析每个 MIDI 文件的音轨...")
    print("="*80)

    for midi_file in midi_files:
        analyze_midi_file(midi_file)

    print("\n" + "="*80)
    print("✅ 分析完成！请根据 '💡 推荐主旋律音轨' 选择要转换的音轨。")
    print("💡 下一步：修改转换脚本，只处理推荐的音轨编号。")

if __name__ == "__main__":
    main()