--TEST--
array copy-on-write vector 019
--FILE--
<?php
$original = [20, 41, 62];
$copy = $original;
$copy[1] = 144; $copy[] = 145;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
20,41,62:20,144,62,145
