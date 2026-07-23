--TEST--
array copy-on-write vector 018
--FILE--
<?php
$original = [19, 39, 59];
$copy = $original;
$copy[1] = 137; $copy[] = 138;
echo implode(',', $original), ':', implode(',', $copy);
--EXPECT--
19,39,59:19,137,59,138
