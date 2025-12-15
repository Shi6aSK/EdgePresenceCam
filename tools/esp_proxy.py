#!/usr/bin/env python3
"""
ESP32 -> OpenAI relay proxy

Usage:
  pip install flask requests
  python esp_proxy.py --key sk-... --port 5000

This starts a simple HTTP server listening on 0.0.0.0:<port> and accepts POST /upload.
It expects a multipart/form-data file field named 'file' (the ESP sends that). The proxy
forwards the file to OpenAI's /v1/audio/transcriptions endpoint using the provided key
and returns the OpenAI JSON response back to the ESP.

Note: Keep your API key secret. This proxy is for local testing only.
"""
from flask import Flask, request, jsonify
import requests
import argparse
import sys

app = Flask(__name__)

OPENAI_URL = 'https://api.openai.com/v1/audio/transcriptions'

parser = argparse.ArgumentParser()
parser.add_argument('--key', required=True, help='OpenAI API key')
parser.add_argument('--port', type=int, default=5000, help='port to listen on')
parser.add_argument('--host', default='0.0.0.0', help='host to bind')
args = parser.parse_args()

if not args.key.startswith('sk-'):
    print('Warning: API key does not look like an OpenAI key (sk-)')

@app.route('/upload', methods=['POST'])
def upload():
    # Accept either multipart with file field 'file' or raw body
    files = None
    data = {'model': request.form.get('model', 'whisper-1')}
    if 'file' in request.files:
        f = request.files['file']
        # read file content into memory (should be small)
        content = f.read()
        files = {'file': (f.filename or 'latest.wav', content, f.mimetype or 'audio/wav')}
        # persist upload to disk for inspection
        try:
            import os
            os.makedirs('uploads', exist_ok=True)
            fn = os.path.join('uploads', f.filename or 'latest.wav')
            with open(fn, 'wb') as wf: wf.write(content)
            print(f"[proxy] saved upload -> {fn} ({len(content)} bytes)")
        except Exception as e:
            print('[proxy] save failed:', e)
    else:
        # try to read raw body
        content = request.get_data()
        if not content or len(content) == 0:
            return jsonify({'error': 'no file provided'}), 400
        files = {'file': ('latest.wav', content, 'audio/wav')}

    headers = {'Authorization': f'Bearer {args.key}'}
    try:
        print('[proxy] forwarding to OpenAI...')
        r = requests.post(OPENAI_URL, headers=headers, files=files, data=data, timeout=60)
    except Exception as e:
        print('[proxy] forward error:', e)
        return jsonify({'error': 'forward_failed', 'detail': str(e)}), 502

    # attempt to parse JSON, but always return JSON to the client
    try:
        j = r.json()
        print('[proxy] forwarded, status', r.status_code)
        return jsonify({'forward_status': r.status_code, 'openai': j}), r.status_code
    except Exception:
        # return raw text wrapped in JSON
        txt = r.text
        print('[proxy] forwarded (non-json), status', r.status_code)
        return jsonify({'forward_status': r.status_code, 'openai_raw': txt}), r.status_code


@app.route('/', methods=['GET'])
def index():
    # Simple health/status page
    try:
        import os
        uploads = []
        updir = os.path.join(os.getcwd(), 'uploads')
        if os.path.isdir(updir):
            for fn in sorted(os.listdir(updir))[-20:]:
                uploads.append(fn)
        return jsonify({'status': 'ok', 'saved_uploads': uploads})
    except Exception as e:
        return jsonify({'status': 'error', 'detail': str(e)}), 500

if __name__ == '__main__':
    print(f'ESP->OpenAI proxy starting on {args.host}:{args.port} (forwarding to OpenAI)')
    print('POST /upload with multipart file field `file`')
    app.run(host=args.host, port=args.port)
