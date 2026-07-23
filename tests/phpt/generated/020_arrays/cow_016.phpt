--TEST--
array copy-on-write vector 016
--FILE--
<?php
$original = [17, 35, 53];
$copy = $original;
$copy[1] = 123; $copy[] = 124;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
17,35,53:17,123,53,124
