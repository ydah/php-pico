--TEST--
array copy-on-write vector 011
--FILE--
<?php
$original = [12, 25, 38];
$copy = $original;
$copy[1] = 88; $copy[] = 89;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
12,25,38:12,88,38,89
