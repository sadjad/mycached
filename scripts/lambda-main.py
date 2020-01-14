import subprocess

def handler(event, context):
    return {'output': subprocess.check_output(["./binary"]).decode('utf-8')}
