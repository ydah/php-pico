--TEST--
array copy-on-write vector 043
--FILE--
<?php
$original = [44, 89, 134];
$copy = $original;
$copy[1] = 312; $copy[] = 313;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
44,89,134:44,312,134,313
