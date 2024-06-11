import sys
import re

log_path = sys.argv[1]

def get_task_addr(line):
    # 0x7fc2c1694328
    res = re.search('0x[0-9a-f]+', line)
    if res:
        return res.group(0)
    return None

with open(log_path, 'r') as fp:
    lines = fp.readlines()
    rep = {}
    serialize = {}
    deserialize = {}
    running = {}
    server_complete = {}
    signal_complete = {}
    complete = {}

    rep_lines = []
    serialize_lines = []
    deserialize_lines = []
    running_lines = []
    server_complete_lines = []
    signal_complete_lines = []
    complete_lines = []

    for line in lines:
        if '[TASK_CHECK]' not in line:
            continue
        task_addr = get_task_addr(line)
        if task_addr is None:
            continue
        if 'Replicated task' in line:
            rep[task_addr] = line
            rep_lines.append(line)
        elif 'Serializing rep_task' in line:
            serialize[task_addr] = line
            serialize_lines.append(line)
        elif 'Deserialized rep_task' in line:
            deserialize[task_addr] = line
            deserialize_lines.append(line)
        elif 'Running rep_task' in line:
            running[task_addr] = line
            running_lines.append(line)
        elif 'Signal complete' in line:
            signal_complete[task_addr] = line
            signal_complete_lines.append(line)
        elif 'Server completed rep_task' in line:
            if task_addr not in running:
                continue
            server_complete[task_addr] = line
            server_complete_lines.append(line)
        elif 'Completing rep_task' in line:
            complete[task_addr] = line
            complete_lines.append(line)

print(f'Sizes: rep={len(rep)}, serialize={len(serialize)}, deserialize={len(deserialize)}, running={len(running)}, '
      f'server_complete={len(server_complete)}, signal_complete={len(signal_complete)}, complete={len(complete)}')
print(f'Line Counts: rep={len(rep_lines)}, serialize={len(serialize_lines)}, deserialize={len(deserialize_lines)}, '
      f'running={len(running_lines)}, server_complete={len(server_complete_lines)}, '
      f'signal_complete={len(signal_complete_lines)}, complete={len(complete_lines)}')
srl_dsrl_diff = set(serialize.keys()) - set(deserialize.keys())
print(f'Diffs: {srl_dsrl_diff}')
# dsrl_complete_diff = set(deserialize.keys()) - set(server_complete.keys())
# print(f'Diffs: {dsrl_complete_diff}')