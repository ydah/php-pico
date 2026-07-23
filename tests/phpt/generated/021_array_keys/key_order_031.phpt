--TEST--
associative key order vector 031
--FILE--
<?php
$items = ['left' => 41, 4 => 113, 'right' => 185];
unset($items[4]);
$items['tail'] = 186;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:41,185,186:1
