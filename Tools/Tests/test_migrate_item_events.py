"""物品事件迁移：历史归因不变、空来源补齐、未来版本拒绝。"""
import unittest
from Tools.migrate_item_events import migrate_record


class ItemEventMigrationTests(unittest.TestCase):
    def test_preserves_history_and_input(self):
        original = {'schemaVersion': 1, 'context': {'eventId': 17, 'rootEventId': 3},
                    'source': {'abilityDefinitionId': 'CombatAbility:bolt'}, 'appliedAmount': 42}
        result = migrate_record(original)
        self.assertEqual(result['schemaVersion'], 2)
        self.assertEqual(result['context'], original['context'])
        self.assertEqual(result['appliedAmount'], 42)
        self.assertEqual(result['source']['abilityDefinitionId'], 'CombatAbility:bolt')
        self.assertEqual(result['source']['itemHandle']['key']['id'], 0)
        self.assertNotIn('itemHandle', original['source'])

    def test_pascal_and_idempotence(self):
        result = migrate_record({'SchemaVersion': 1, 'Source': {}})
        self.assertEqual(result['ItemQuantity'], 0)
        self.assertEqual(result, migrate_record(result))

    def test_future_missing_and_malformed_are_rejected(self):
        for record in ({'schemaVersion': 3}, {}, {'schemaVersion': True}, {'schemaVersion': 1, 'source': []}):
            with self.assertRaises(ValueError):
                migrate_record(record)


if __name__ == '__main__':
    unittest.main()
