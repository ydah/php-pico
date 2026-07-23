--TEST--
array copy-on-write vector 013
--FILE--
<?php
$original = [14, 29, 44];
$copy = $original;
$copy[1] = 102; $copy[] = 103;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
14,29,44:14,102,44,103
