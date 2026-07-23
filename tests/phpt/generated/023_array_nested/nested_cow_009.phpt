--TEST--
nested array copy-on-write vector 009
--FILE--
<?php
$tree = ['node' => ['value' => 37, 'items' => [38]]];
$copy = $tree;
$copy['node']['value'] += 4;
$copy['node']['items'][] = 43;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
37:41:38:38,43
