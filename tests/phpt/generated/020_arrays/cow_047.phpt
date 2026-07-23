--TEST--
array copy-on-write vector 047
--FILE--
<?php
$original = [48, 97, 146];
$copy = $original;
$copy[1] = 340; $copy[] = 341;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
48,97,146:48,340,146,341
