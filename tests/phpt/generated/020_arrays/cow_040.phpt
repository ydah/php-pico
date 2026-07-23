--TEST--
array copy-on-write vector 040
--FILE--
<?php
$original = [41, 83, 125];
$copy = $original;
$copy[1] = 291; $copy[] = 292;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
41,83,125:41,291,125,292
