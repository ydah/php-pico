--TEST--
array copy-on-write vector 007
--FILE--
<?php
$original = [8, 17, 26];
$copy = $original;
$copy[1] = 60; $copy[] = 61;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
8,17,26:8,60,26,61
