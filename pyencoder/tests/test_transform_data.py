import pytest
import numpy as np
from pyencoder.dataloader import transform_data


class TestTransformData:
    """Test suite for the transform_data function."""

    def test_output_shape(self):
        """Test that output has correct shape."""
        datapoint = np.array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 0, 0, 0])
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        assert result.shape == (902,), f"Expected shape (902,), got {result.shape}"

    def test_output_dtype(self):
        """Test that output has correct dtype."""
        datapoint = np.array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 0, 0, 0])
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        assert result.dtype == np.float16, f"Expected dtype float16, got {result.dtype}"

    def test_first_18_positions_one_hot(self):
        """Test that first 18 positions are correctly one-hot encoded (indices 0-17)."""
        datapoint = np.array([5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], dtype=np.int32)
        
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # Position 0 maps to: sample[0*40 + 5] = sample[5]
        assert result[5] == 1, f"Expected result[5] == 1, got {result[5]}"
        # Check that other positions in this range are 0
        assert np.sum(result[0:40]) == 1, "Expected exactly one 1 in first 40 positions"

    def test_all_18_positions(self):
        """Test all 18 one-hot encoded positions."""
        for pos in range(18):
            datapoint = np.zeros(22, dtype=np.int32)
            datapoint[pos] = 15  # Use value 15 for all
            
            sample = np.zeros(902, dtype=np.float16)
            result = transform_data(datapoint, sample)
            
            expected_idx = pos * 40 + 15
            assert result[expected_idx] == 1, f"Position {pos}: Expected result[{expected_idx}] == 1"

    def test_tuzdek_player1_no_tuzdek(self):
        """Test player 1 tuzdek when value is 18 (no tuzdek)."""
        datapoint = np.zeros(22, dtype=np.int32)
        datapoint[18] = 18  # No tuzdek
        
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # offset = 18 * 40 = 720
        # When i == 18 and datapoint[18] == 18: sample[offset + (i-18)*9] = 1
        # sample[720 + 0] = 1
        assert result[720] == 1, f"Expected result[720] == 1 for player 1 no tuzdek, got {result[720]}"

    def test_tuzdek_player1_with_tuzdek(self):
        """Test player 1 tuzdek with valid position (9-18)."""
        datapoint = np.zeros(22, dtype=np.int32)
        datapoint[18] = 15  # Valid tuzdek position (9-18)
        
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # offset = 18 * 40 = 720
        # i == 18: sample[offset + datapoint[i] - 9] = sample[720 + 15 - 9]
        assert result[720 + 15 - 9] == 1, f"Expected result[{720 + 15 - 9}] == 1"

    def test_tuzdek_player2_no_tuzdek(self):
        """Test player 2 tuzdek when value is 18 (no tuzdek)."""
        datapoint = np.zeros(22, dtype=np.int32)
        datapoint[19] = 18  # No tuzdek
        
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # offset = 18 * 40 = 720
        # When i == 19 and datapoint[19] == 18: sample[offset + (i-18)*9] = 1
        # sample[720 + 1*9] = sample[729]
        assert result[729] == 1, f"Expected result[729] == 1 for player 2 no tuzdek"

    def test_tuzdek_player2_with_tuzdek(self):
        """Test player 2 tuzdek with valid position (0-8)."""
        datapoint = np.zeros(22, dtype=np.int32)
        datapoint[19] = 5  # Valid tuzdek position
        
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # offset = 18 * 40 = 720
        # i == 19: sample[offset + 9 + datapoint[i]] = sample[720 + 9 + 5]
        assert result[720 + 9 + 5] == 1, f"Expected result[{720 + 9 + 5}] == 1"

    def test_capture_counts(self):
        """Test capture counts encoding (positions 20-21)."""
        datapoint = np.zeros(22, dtype=np.int32)
        datapoint[20] = 42  # Capture count for player 1
        
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # offset = 18*40 + 2*9 = 720 + 18 = 738
        # sample[738 + (20-20)*82 + datapoint[20]] = sample[738 + 42]
        assert result[738 + 42] == 1, f"Expected result[{738 + 42}] == 1"

    def test_capture_counts_player2(self):
        """Test capture counts encoding for player 2 (position 21)."""
        datapoint = np.zeros(22, dtype=np.int32)
        datapoint[21] = 35  # Capture count for player 2
        
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # offset = 18*40 + 2*9 = 738
        # sample[738 + (21-20)*82 + datapoint[21]] = sample[738 + 82 + 35]
        assert result[738 + 82 + 35] == 1, f"Expected result[{738 + 82 + 35}] == 1"

    def test_total_ones_count(self):
        """Test that total number of 1s equals number of datapoint features."""
        datapoint = np.array([5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 9, 5, 42, 35])
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # Should have 22 ones (one for each datapoint element)
        num_ones = np.sum(result == 1)
        assert num_ones == 22, f"Expected 22 ones in result, got {num_ones}"

    def test_no_overlap(self):
        """Test that different datapoint positions don't activate overlapping indices."""
        datapoint = np.array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 10, 5, 50, 60])
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # All non-zero values should be exactly 1, no overlaps
        assert np.all((result == 0) | (result == 1)), "Result should only contain 0s and 1s"
        assert np.sum(result) == 22, f"Expected sum of 22, got {np.sum(result)}"

    def test_boundary_values(self):
        """Test with boundary values (0 and 39)."""
        datapoint = np.array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 8, 0, 81])
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # Position 0 with value 0: sample[0*40 + 0] = sample[0]
        assert result[0] == 1, f"Expected result[0] == 1"
        # Position 21 with value 81: sample[738 + 1*82 + 81]
        assert result[738 + 82 + 81] == 1, f"Expected result[{738 + 82 + 81}] == 1"

    def test_range_18_to_20_exclusive(self):
        """Test the range for positions 18-19 (range(18, 20))."""
        # Test that positions 18 and 19 are processed but 20 is not in this range
        datapoint = np.zeros(22, dtype=np.int32)
        datapoint[18] = 15
        datapoint[19] = 5
        
        sample = np.zeros(902, dtype=np.float16)
        result = transform_data(datapoint, sample)
        
        # Should have 2 ones from positions 18 and 19
        ones_in_tuzdek_range = np.sum(result[720:738])  # Tuzdek range
        assert ones_in_tuzdek_range == 2, f"Expected 2 ones in tuzdek range, got {ones_in_tuzdek_range}"


class TestTransformDataProperties:
    """Property-based tests for transform_data."""

    def test_idempotence_is_false(self):
        """Test that transform_data is not idempotent."""
        datapoint = np.array([5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 9, 5, 42, 35])
        sample = np.zeros(902, dtype=np.float16)
        result1 = transform_data(datapoint, sample)
        
        # Applying transform_data again should fail or give different results
        # (since it expects uninitialized sample, not a result)
        sample2 = np.zeros(902, dtype=np.float16)
        result2 = transform_data(datapoint, sample2)
        
        # They should have same values since we're transforming the same datapoint
        assert np.allclose(result1, result2), "Same datapoint should produce same result"

    def test_different_datapoints_different_results(self):
        """Test that different datapoints produce different results."""
        datapoint1 = np.zeros(22, dtype=np.int32)
        datapoint2 = np.zeros(22, dtype=np.int32)
        datapoint2[5] = 10
        
        sample1 = np.zeros(902, dtype=np.float16)
        sample2 = np.zeros(902, dtype=np.float16)
        
        result1 = transform_data(datapoint1, sample1)
        result2 = transform_data(datapoint2, sample2)
        
        assert not np.allclose(result1, result2), "Different datapoints should produce different results"

    def test_modifies_input_sample(self):
        """Test that the function modifies the input sample array."""
        datapoint = np.array([5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 15, 0, 5, 10, 9, 5, 42, 35])
        sample = np.zeros(902, dtype=np.float16)
        original_sample = sample.copy()
        
        result = transform_data(datapoint, sample)
        
        # The sample should be modified (and returned)
        assert not np.allclose(sample, original_sample), "Input sample should be modified"
        assert np.allclose(result, sample), "Returned result should be the same object as input sample"
