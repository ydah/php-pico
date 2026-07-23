--TEST--
array copy-on-write vector 028
--FILE--
<?php
$original = [29, 59, 89];
$copy = $original;
$copy[1] = 207; $copy[] = 208;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
29,59,89:29,207,89,208
