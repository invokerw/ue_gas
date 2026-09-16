"""将 Combat v1/v2 事件 JSON 离线升级到经济契约 v3，保留原记录与因果身份。"""
import argparse
import copy
import json
from pathlib import Path


def migrate_record(record):
    """接受 UE JSON Converter 的 camelCase 或反射字段名；未知版本失败关闭。"""
    if not isinstance(record, dict):
        raise ValueError('Each event must be an object')
    result = copy.deepcopy(record)
    pascal = 'SchemaVersion' in result
    key = lambda name: name if pascal else name[0].lower() + name[1:]
    version = result.get(key('SchemaVersion'))
    if type(version) is not int or version not in (1, 2, 3):
        raise ValueError(f'Unsupported event schema: {version!r}')
    if version == 3:
        return result
    if version == 1:
        source = result.setdefault(key('Source'), {})
        if not isinstance(source, dict):
            raise ValueError('Event source must be an object')
        source[key('ItemDefinitionId')] = ''
        source[key('ItemHandle')] = {key('Key'): {key('Id'): 0, key('Generation'): 0, key('LifeGeneration'): 0}}
        result[key('ItemAction')] = 'None'
        result[key('ItemQuantity')] = 0
        result[key('ItemCharges')] = 0
    result[key('GoldDelta')] = 0
    result[key('GoldBalance')] = 0
    result[key('SchemaVersion')] = 3
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data = json.loads(args.input.read_text(encoding='utf-8-sig'))
    migrated = [migrate_record(x) for x in data] if isinstance(data, list) else migrate_record(data)
    # 输出采用独占创建；输入或已有迁移结果不会被覆盖。
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(migrated, stream, ensure_ascii=False, indent=2)
        stream.write('\n')


if __name__ == '__main__':
    main()
