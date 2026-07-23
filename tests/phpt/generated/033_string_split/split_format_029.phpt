--TEST--
string split transform and padding vector 029
--FILE--
<?php
$parts = explode(',', 'v29,v30,v31');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX29'), ':', str_pad($parts[0], 6, '-');
--EXPECT--
v29|v30|v31:3:V30:mix29:v29---
