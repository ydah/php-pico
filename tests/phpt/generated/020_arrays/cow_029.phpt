--TEST--
array copy-on-write vector 029
--FILE--
<?php
$original = [30, 61, 92];
$copy = $original;
$copy[1] = 214; $copy[] = 215;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
30,61,92:30,214,92,215
