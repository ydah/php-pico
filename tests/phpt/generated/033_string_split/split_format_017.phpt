--TEST--
string split transform and padding vector 017
--FILE--
<?php
$parts = explode(',', 'v17,v18,v19');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX17'), ':', str_pad($parts[0], 6, '-');
--EXPECT--
v17|v18|v19:3:V18:mix17:v17---
