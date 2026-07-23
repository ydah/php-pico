--TEST--
nested array copy-on-write vector 012
--FILE--
<?php
$tree = ['node' => ['value' => 49, 'items' => [50]]];
$copy = $tree;
$copy['node']['value'] += 7;
$copy['node']['items'][] = 58;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
49:56:50:50,58
