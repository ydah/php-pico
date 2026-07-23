--TEST--
array copy-on-write vector 025
--FILE--
<?php
$original = [26, 53, 80];
$copy = $original;
$copy[1] = 186; $copy[] = 187;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
26,53,80:26,186,80,187
