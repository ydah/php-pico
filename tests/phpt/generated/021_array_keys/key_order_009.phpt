--TEST--
associative key order vector 009
--FILE--
<?php
$items = ['left' => 19, 4 => 47, 'right' => 75];
unset($items[4]);
$items['tail'] = 76;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:19,75,76:1
