# SPDX-License-Identifier: GPL-3.0-or-later
"""Acceptance guards for the observation comparator; synthetic fixtures only."""
import unittest
from compare_mohawk_v5_observations import compare


def receipt(rows):
    return dict(kind='synthetic test fixture, not runtime evidence', results=rows)


def observed(name='a', actual=1):
    return dict(name=name, status='observed', actual=actual, expected=1)


class ComparisonGuards(unittest.TestCase):
    def test_match(self):
        self.assertTrue(compare(receipt([observed()]), receipt([observed()]))['passed'])

    def test_static_expectation_cannot_hide_actual_mismatch(self):
        result = compare(receipt([observed()]), receipt([observed(actual=0)]))
        self.assertFalse(result['passed'])
        self.assertEqual(result['results'][0]['status'], 'mismatch')

    def test_missing_extra_observations_fail(self):
        self.assertFalse(compare(receipt([observed()]), receipt([observed(), observed('extra')]))['passed'])

    def test_faults_are_not_equal_values(self):
        fault = dict(name='a', status='exception', exception='0xc0000094')
        self.assertFalse(compare(receipt([fault]), receipt([fault]))['passed'])

    def test_missing_actual_is_not_success(self):
        row = dict(name='a', status='observed', expected=1)
        self.assertFalse(compare(receipt([row]), receipt([row]))['passed'])

    def test_duplicate_cases_rejected(self):
        with self.assertRaises(ValueError):
            compare(receipt([observed(), observed()]), receipt([observed()]))

    def test_different_expression_is_not_a_match(self):
        left, right = observed(), observed()
        left['expression_sha256'] = 'expression-a'
        right['expression_sha256'] = 'expression-b'
        self.assertFalse(compare(receipt([left]), receipt([right]))['passed'])

    def test_empty_receipt_rejected(self):
        with self.assertRaises(ValueError):
            compare(receipt([]), receipt([]))


if __name__ == '__main__': unittest.main()
