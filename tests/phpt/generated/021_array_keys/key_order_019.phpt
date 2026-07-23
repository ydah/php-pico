--TEST--
associative key order vector 019
--FILE--
<?php
$items = ['left' => 29, 4 => 77, 'right' => 125];
unset($items[4]);
$items['tail'] = 126;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:29,125,126:1
