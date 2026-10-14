#!/usr/bin/env python3
"""Convert the supplied GC Sofdec films for Android MediaPlayer.

Requires ffmpeg/ffprobe on the development host; never downloads game data.
Usage: python3 tools/prepare_android_movies.py /path/to/disc1/files
Use --add-mono-only to retain the existing MP4 video/stereo packets and add
the original ADX sound downmixed to a separate mono AAC track.
The MP4s are derived from the user's imported disc, not replacement artwork.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def probe(path):
    return json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-show_entries',
        'format=duration:stream=index,codec_type,codec_name,width,height,sample_rate,channels,channel_layout,sample_aspect_ratio,duration,start_time'
        ':stream_tags=title,handler_name,language:stream_disposition=default',
        '-of', 'json', str(path)]))


def stream_digest(path, selector):
    # Hash encoded packets, so a remux cannot silently re-encode video/stereo.
    return subprocess.check_output([
        'ffmpeg', '-hide_banner', '-loglevel', 'error', '-i', str(path),
        '-map', selector, '-c', 'copy', '-f', 'hash', '-hash', 'sha256', '-'],
        text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc_files', type=Path)
    parser.add_argument('--output', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'app/src/main/assets/movies')
    parser.add_argument('--add-mono-only', action='store_true',
                        help='Copy existing video/stereo; encode only the new mono track from original ADX.')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    records = []
    for name in ('opening', 'demo0', 'demo1'):
        source = args.disc_files / 'Movie' / (name + '.sfd')
        if not source.is_file():
            raise SystemExit('Original disc film missing: ' + str(source))
        destination = args.output / (name + '.mp4')
        temporary = destination.with_suffix('.tmp.mp4')
        command = ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y']
        before = None
        if args.add_mono_only:
            if not destination.is_file():
                raise SystemExit('Existing MP4 missing for --add-mono-only: ' + str(destination))
            before = {selector: stream_digest(destination, selector)
                      for selector in ('0:v:0', '0:a:0')}
            command += ['-i', str(destination), '-i', str(source),
                        '-map', '0:v:0', '-map', '0:a:0', '-map', '1:a:0',
                        '-c', 'copy', '-c:a:1', 'aac', '-b:a:1', '64k', '-ac:a:1', '1']
        else:
            command += ['-i', str(source), '-map', '0:v:0', '-map', '0:a:0', '-map', '0:a:0',
                        '-c:v', 'libx264', '-preset', 'medium', '-crf', '18',
                        '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-b:a:0', '128k', '-ac:a:0', '2',
                        '-b:a:1', '64k', '-ac:a:1', '1']
        # MP4 exposes track names through handler_name; title is also written
        # for muxers/readers that expose the per-stream title field.
        command += ['-metadata:s:a:0', 'title=Stereo', '-metadata:s:a:0', 'handler_name=Stereo',
                    '-metadata:s:a:1', 'title=Mono', '-metadata:s:a:1', 'handler_name=Mono',
                    '-disposition:a:0', 'default', '-disposition:a:1', '0',
                    '-movflags', '+faststart', str(temporary)]
        try:
            subprocess.run(command, check=True)
            result = probe(temporary)
            audio = [stream for stream in result['streams'] if stream['codec_type'] == 'audio']
            if len(audio) != 2 or [stream.get('channels') for stream in audio] != [2, 1]:
                raise SystemExit(name + ': expected stereo track first and mono track second')
            if [stream['disposition']['default'] for stream in audio] != [1, 0]:
                raise SystemExit(name + ': incorrect audio track default dispositions')
            if before is not None:
                for selector, original in before.items():
                    if stream_digest(temporary, selector) != original:
                        raise SystemExit(name + ': copied encoded stream changed: ' + selector)
            temporary.replace(destination)
        finally:
            temporary.unlink(missing_ok=True)
        records.append({'name': name, 'source': 'Movie/' + source.name,
                        'source_sha256': digest(source),
                        'mp4_sha256': digest(destination), 'bytes': destination.stat().st_size,
                        'probe': result})
        print(name + (': preserved video/stereo; added original mono sound'
                      if args.add_mono_only else ': converted original film with stereo/mono sound'), flush=True)
    (args.output / 'manifest.json').write_text(json.dumps({
        'format': 2, 'video_encoding': 'H264 CRF18; decoded original frames and timing',
        'audio_encoding': 'AAC stereo 128kbps (default first track) + AAC mono 64kbps (second track); decoded original ADX',
        'audio_tracks': [{'audio_stream': 0, 'title': 'Stereo', 'channels': 2, 'default': True},
                         {'audio_stream': 1, 'title': 'Mono', 'channels': 1, 'default': False}],
        'movies': records}, indent=2) + '\n')


if __name__ == '__main__':
    main()
