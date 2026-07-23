--TEST--
array copy-on-write vector 046
--FILE--
<?php
$original = [47, 95, 143];
$copy = $original;
$copy[1] = 333; $copy[] = 334;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
47,95,143:47,333,143,334
