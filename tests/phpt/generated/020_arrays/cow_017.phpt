--TEST--
array copy-on-write vector 017
--FILE--
<?php
$original = [18, 37, 56];
$copy = $original;
$copy[1] = 130; $copy[] = 131;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
18,37,56:18,130,56,131
