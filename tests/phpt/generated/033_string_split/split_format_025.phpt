--TEST--
string split transform and padding vector 025
--FILE--
<?php
$parts = explode(',', 'v25,v26,v27');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX25'), ':', str_pad($parts[0], 6, '-');
--EXPECT--
v25|v26|v27:3:V26:mix25:v25---
