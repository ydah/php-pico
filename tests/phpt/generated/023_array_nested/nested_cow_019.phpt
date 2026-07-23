--TEST--
nested array copy-on-write vector 019
--FILE--
<?php
$tree = ['node' => ['value' => 77, 'items' => [78]]];
$copy = $tree;
$copy['node']['value'] += 7;
$copy['node']['items'][] = 86;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
77:84:78:78,86
