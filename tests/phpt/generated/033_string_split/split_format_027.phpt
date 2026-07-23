--TEST--
string split transform and padding vector 027
--FILE--
<?php
$parts = explode(',', 'v27,v28,v29');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX27'), ':', str_pad($parts[0], 8, '-');
--EXPECT--
v27|v28|v29:3:V28:mix27:v27-----
