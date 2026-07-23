--TEST--
array copy-on-write vector 027
--FILE--
<?php
$original = [28, 57, 86];
$copy = $original;
$copy[1] = 200; $copy[] = 201;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
28,57,86:28,200,86,201
