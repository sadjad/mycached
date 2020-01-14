import subprocess

def handler(event, context):
    event = [str(x) for x in event]
    return {'output': subprocess.check_output(["./binary"] + event).decode('utf-8')}
