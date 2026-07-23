--TEST--
array copy-on-write vector 015
--FILE--
<?php
$original = [16, 33, 50];
$copy = $original;
$copy[1] = 116; $copy[] = 117;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
16,33,50:16,116,50,117
