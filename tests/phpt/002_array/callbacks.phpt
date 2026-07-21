--TEST--
array callbacks and copy-on-write sorting
--FILE--
<?php
$original = [3, 1, 2];
$sorted = $original;
sort($sorted);
$mapped = array_map(fn($value) => $value * 2, $sorted);
$filtered = array_filter($mapped, fn($value) => $value > 2);
echo implode(',', $original), ':', implode(',', $filtered), ':';
echo array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
--EXPECT--
3,1,2:4,6:10
