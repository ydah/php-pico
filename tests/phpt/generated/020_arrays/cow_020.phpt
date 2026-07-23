--TEST--
array copy-on-write vector 020
--FILE--
<?php
$original = [21, 43, 65];
$copy = $original;
$copy[1] = 151; $copy[] = 152;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
21,43,65:21,151,65,152
